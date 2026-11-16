// Reference: tgsm/pbg src/rwsdk/src/baframe.c
/*
 * baframe.c: RenderWare frames. A frame holds a modelling matrix relative to its parent and the
 * LTM built from the hierarchy; a hierarchy whose frames moved sits in the dirty frame list until
 * it is synchronised. The frame module's free list hands out the frames. RwFrameScale and
 * RwFrameCount have no copy in retail (dead-stripped).
 * Retail inlines no call here (built with a per-object -inline noauto, docs/exceptions.md).
 */
#include <rwsdk/badevice.h>
#include <rwsdk/baframe.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/batkreg.h>

/* The free list of frames; RwFreeListCreateAndPreallocateSpace builds it in place here. */
static RwFreeList frameFreeList;

/* The frame module's registration (offset of its globals in the engine instance). */
static RwModuleInfo frameModule;

/* Frames per free-list block, and blocks allocated when the module opens. */
static RwInt32 _rwFrameFreeListBlockSize = 50;
static RwInt32 _rwFrameFreeListPreallocBlocks = 1;

/* The frame module's globals (RWFRAMEGLOBAL invented). */
typedef struct rwFrameGlobals {
    RwFreeList* frames; /* 0x0 */
} rwFrameGlobals;

#define RWFRAMEGLOBAL(var) \
    (RWPLUGINOFFSET(rwFrameGlobals, RwEngineInstance, frameModule.globalsOffset)->var)

/* The plugins registered with frames: no plugin data yet, so a frame is just an RwFrame. */
RwPluginRegistry frameTKList = {
    sizeof(RwFrame),
    sizeof(RwFrame),
    0,
    0,
    NULL,
    NULL,
};

/* Sets the frame free list's block size and preallocated blocks, for the next time it opens. */
void RwFrameSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    _rwFrameFreeListBlockSize = blockSize;
    _rwFrameFreeListPreallocBlocks = numBlocksToPrealloc;
} // 0x80294344

/* Opens the frame module: creates the frame free list and empties the dirty frame list. */
void* _rwFrameOpen(void* instance, RwInt32 offset, RwInt32 size) {
    frameModule.globalsOffset = offset;

    RWFRAMEGLOBAL(frames) = RwFreeListCreateAndPreallocateSpace(
        frameTKList.sizeOfStruct, _rwFrameFreeListBlockSize, sizeof(RwUInt32),
        _rwFrameFreeListPreallocBlocks, &frameFreeList, rwMEMHINTDUR_GLOBAL | rwID_FRAMELIST);
    if (RWFRAMEGLOBAL(frames) == NULL) {
        return NULL;
    }

    rwLinkListInitialize(&RWSRCGLOBAL(dirtyFrameList));

    frameModule.numInstances++;

    return instance;
} // 0x80294350

/* Closes the frame module: destroys the frame free list. */
void* _rwFrameClose(void* instance, RwInt32 offset, RwInt32 size) {
    if (RWFRAMEGLOBAL(frames) != NULL) {
        RwFreeListDestroy(RWFRAMEGLOBAL(frames));
        RWFRAMEGLOBAL(frames) = NULL;
    }

    frameModule.numInstances--;

    return instance;
} // 0x802943F4

/* Makes root the root of frame and of every frame below it. */
static void rwSetHierarchyRoot(RwFrame* frame, RwFrame* root) {
    frame->root = root;
    for (frame = frame->child; frame != NULL; frame = frame->next) {
        rwSetHierarchyRoot(frame, root);
    }
} // 0x80294458

/*
 * Clones frame and the frames below it, the clones' hierarchy rooted at root (the new clone when
 * root is NULL). Each original's root is pointed at its clone, so that objects can find the clone
 * of their frame until _rwFramePurgeClone restores the originals.
 */
static RwFrame* rwFrameCloneRecurse(RwFrame* frame, RwFrame* root) {
    RwFrame* clone;
    RwFrame* child;

    clone = (RwFrame*)RwFreeListAlloc(RWFRAMEGLOBAL(frames), rwMEMHINTDUR_EVENT | rwID_FRAMELIST);
    if (clone == NULL) {
        return NULL;
    }

    if (root == NULL) {
        root = clone;
    }

    rwObjectCopy(clone, frame);
    rwLinkListInitialize(&clone->objectList);
    rwObjectSetParent(clone, NULL);

    clone->modelling = frame->modelling;
    clone->child = NULL;
    clone->next = NULL;
    clone->root = root;
    frame->root = clone;

    for (child = frame->child; child != NULL; child = child->next) {
        RwFrame* childClone = rwFrameCloneRecurse(child, root);

        if (childClone == NULL) {
            RwFrameDestroyHierarchy(clone);
            return NULL;
        }

        childClone->next = clone->child;
        clone->child = childClone;
        rwObjectSetParent(childClone, clone);
    }

    _rwPluginRegistryInitObject(&frameTKList, clone);
    _rwPluginRegistryCopyObject(&frameTKList, clone, frame);

    return clone;
} // 0x802944AC

/* Clones the hierarchy below root, leaving each original linked to its clone. */
RwFrame* _rwFrameCloneAndLinkClones(RwFrame* root) {
    RwFrame* clone = rwFrameCloneRecurse(root, NULL);

    if (clone != NULL) {
        rwObjectSetPrivateFlags(clone, rwObjectGetPrivateFlags(clone) & ~rwFRAMEPRIVATEHIERARCHYSYNC);
        RwFrameUpdateObjects(clone);
    }

    return clone;
} // 0x80294660

/* Restores the roots of the hierarchy below root after _rwFrameCloneAndLinkClones. */
RwFrame* _rwFramePurgeClone(RwFrame* root) {
    RwFrame* parent = RwFrameGetParent(root);

    if (parent != NULL) {
        rwSetHierarchyRoot(root, parent->root);
    } else {
        rwSetHierarchyRoot(root, root);
    }

    return root;
} // 0x802946AC

/* Whether the frame's hierarchy needs synchronising. */
RwBool RwFrameDirty(const RwFrame* frame) {
    return rwObjectTestPrivateFlags(frame->root, rwFRAMEPRIVATEHIERARCHYSYNC);
} // 0x802946F8

/* Initialises a frame: no parent, no children, no objects, identity matrices. */
void _rwFrameInit(RwFrame* frame) {
    rwObjectInitialize(frame, rwFRAME, 0);
    rwLinkListInitialize(&frame->objectList);

    rwMatrixSetFlags(&frame->modelling, rwMATRIXTYPEORTHONORMAL);
    RwMatrixSetIdentity(&frame->modelling);
    rwMatrixSetFlags(&frame->ltm, rwMATRIXTYPEORTHONORMAL);
    RwMatrixSetIdentity(&frame->ltm);

    frame->child = NULL;
    frame->next = NULL;
    frame->root = frame;

    _rwPluginRegistryInitObject(&frameTKList, frame);
} // 0x80294708

/* Creates a frame, with identity matrices and no parent. */
RwFrame* RwFrameCreate(void) {
    RwFrame* frame;

    frame = (RwFrame*)RwFreeListAlloc(RWFRAMEGLOBAL(frames), rwMEMHINTDUR_EVENT | rwID_FRAMELIST);
    if (frame == NULL) {
        return NULL;
    }

    _rwFrameInit(frame);

    return frame;
} // 0x802947CC

/*
 * Releases a frame's plugin data and unlinks it from its parent and the dirty frame list; its
 * children are left without a parent (their roots are left as they were).
 */
void _rwFrameDeInit(RwFrame* frame) {
    RwFrame* child;

    _rwPluginRegistryDeInitObject(&frameTKList, frame);

    if (RwFrameGetParent(frame) != NULL) {
        RwFrameRemoveChild(frame);
    }

    if (rwObjectTestPrivateFlags(frame, rwFRAMEPRIVATEHIERARCHYSYNC)) {
        rwLinkListRemoveLLLink(&frame->inDirtyListLink);
    }

    for (child = frame->child; child != NULL; child = child->next) {
        rwObjectSetParent(child, NULL);
    }
} // 0x8029482C

/* Destroys a frame (not its children). */
RwBool RwFrameDestroy(RwFrame* frame) {
    _rwFrameDeInit(frame);
    RwFreeListFree(RWFRAMEGLOBAL(frames), frame);

    return TRUE;
} // 0x802948B8

/* Releases the plugin data of a frame of a hierarchy being destroyed and unlinks it as dirty. */
static void FrameDestroyRecurseDeInitLeaf(RwFrame* frame) {
    _rwPluginRegistryDeInitObject(&frameTKList, frame);

    if (rwObjectTestPrivateFlags(frame, rwFRAMEPRIVATEHIERARCHYSYNC)) {
        rwLinkListRemoveLLLink(&frame->inDirtyListLink);
    }
} // 0x80294904

/* Destroys a frame of a hierarchy being destroyed. */
static void rwFrameDestroyRecurseDestroyLeaf(RwFrame* frame) {
    FrameDestroyRecurseDeInitLeaf(frame);
    RwFreeListFree(RWFRAMEGLOBAL(frames), frame);
} // 0x80294960

/* Destroys frame and every frame below it, children first. */
static void rwFrameDestroyRecurse(RwFrame* frame) {
    if (frame != NULL) {
        RwFrame* child = frame->child;

        while (child != NULL) {
            RwFrame* next = child->next;

            rwFrameDestroyRecurse(child);
            child = next;
        }

        rwFrameDestroyRecurseDestroyLeaf(frame);
    }
} // 0x802949A8

/* Destroys frame and every frame below it. */
RwBool RwFrameDestroyHierarchy(RwFrame* frame) {
    rwFrameDestroyRecurse(frame);

    return TRUE;
} // 0x80294A04

/*
 * Marks a frame as moved: its hierarchy goes into the dirty frame list (if not there already) so
 * that the next synchronisation rebuilds its LTMs and resyncs its objects.
 */
RwFrame* RwFrameUpdateObjects(RwFrame* frame) {
    RwUInt8 rootFlags = rwObjectGetPrivateFlags(frame->root);

    if (!(rootFlags & rwFRAMEPRIVATEHIERARCHYSYNC)) {
        rwLinkListAddLLLink(&RWSRCGLOBAL(dirtyFrameList), &frame->root->inDirtyListLink);
    }

    rwObjectSetPrivateFlags(frame->root, rootFlags | rwFRAMEPRIVATEHIERARCHYSYNC);
    rwObjectSetPrivateFlags(frame, rwObjectGetPrivateFlags(frame) | rwFRAMEPRIVATESUBTREESYNC);

    return frame;
} // 0x80294A28

/* The frame's LTM, after rebuilding its hierarchy's LTMs if they are out of date. */
RwMatrix* RwFrameGetLTM(RwFrame* frame) {
    if (rwObjectTestPrivateFlags(frame->root, rwFRAMEPRIVATEHIERARCHYSYNCLTM)) {
        _rwFrameSyncHierarchyLTM(frame->root);
    }

    return &frame->ltm;
} // 0x80294A94

/* The root of the frame's hierarchy. */
RwFrame* RwFrameGetRoot(const RwFrame* frame) {
    return frame->root;
} // 0x80294AD8

/*
 * Makes child the first child of parent without marking anything as moved (the frame list stream
 * reader links its frames this way when they are not to be updated).
 */
RwFrame* RwFrameAddChildNoUpdate(RwFrame* parent, RwFrame* child) {
    if (RwFrameGetParent(child) != NULL) {
        RwFrameRemoveChild(child);
    }

    child->next = parent->child;
    parent->child = child;
    rwObjectSetParent(child, parent);
    rwSetHierarchyRoot(child, parent->root);

    return parent;
} // 0x80294AE0

/* Makes child the first child of parent; child leaves its old parent, if any. */
RwFrame* RwFrameAddChild(RwFrame* parent, RwFrame* child) {
    if (RwFrameGetParent(child) != NULL) {
        RwFrameRemoveChild(child);
    }

    child->next = parent->child;
    parent->child = child;
    rwObjectSetParent(child, parent);
    rwSetHierarchyRoot(child, parent->root);

    /* child is no longer a root: take it out of the dirty frame list. */
    if (rwObjectTestPrivateFlags(child, rwFRAMEPRIVATEHIERARCHYSYNC)) {
        rwLinkListRemoveLLLink(&child->inDirtyListLink);
        rwObjectSetPrivateFlags(child, rwObjectGetPrivateFlags(child) & ~rwFRAMEPRIVATEHIERARCHYSYNC);
    }

    RwFrameUpdateObjects(child);

    return parent;
} // 0x80294B48

/* Detaches child from its parent; it becomes the root of its own hierarchy. */
RwFrame* RwFrameRemoveChild(RwFrame* child) {
    RwFrame* current = RwFrameGetParent(child)->child;

    if (current == child) {
        RwFrameGetParent(child)->child = child->next;
    } else {
        while (current->next != child) {
            current = current->next;
        }

        current->next = child->next;
    }

    rwObjectSetParent(child, NULL);
    child->next = NULL;
    rwSetHierarchyRoot(child, child);

    RwFrameUpdateObjects(child);

    return child;
} // 0x80294BE8

/* Calls callBack for each child of frame until it returns NULL. */
RwFrame* RwFrameForAllChildren(RwFrame* frame, RwFrameCallBack callBack, void* data) {
    RwFrame* child = frame->child;

    while (child != NULL) {
        RwFrame* next = child->next;

        if (callBack(child, data) == NULL) {
            return frame;
        }

        child = next;
    }

    return frame;
} // 0x80294C6C

/* Translates the frame's modelling matrix by v. */
RwFrame* RwFrameTranslate(RwFrame* frame, const RwV3d* v, RwOpCombineType combine) {
    RwMatrixTranslate(&frame->modelling, v, combine);
    RwFrameUpdateObjects(frame);

    return frame;
} // 0x80294CF0

/* Transforms the frame's modelling matrix by m. */
RwFrame* RwFrameTransform(RwFrame* frame, const RwMatrix* m, RwOpCombineType combine) {
    RwMatrixTransform(&frame->modelling, m, combine);
    RwFrameUpdateObjects(frame);

    return frame;
} // 0x80294D2C

/* Rotates the frame's modelling matrix by angle degrees about axis. */
RwFrame* RwFrameRotate(RwFrame* frame, const RwV3d* axis, RwReal angle, RwOpCombineType combine) {
    RwMatrixRotate(&frame->modelling, axis, angle, combine);
    RwFrameUpdateObjects(frame);

    return frame;
} // 0x80294D68

/* Resets the frame's modelling matrix to the identity. */
RwFrame* RwFrameSetIdentity(RwFrame* frame) {
    RwMatrixSetIdentity(&frame->modelling);
    RwFrameUpdateObjects(frame);

    return frame;
} // 0x80294DA4

/* Registers a plugin with frames: size bytes of plugin data in every frame. */
RwInt32 RwFrameRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                              RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(&frameTKList, size, pluginID, constructCB, destructCB, copyCB);
} // 0x80294E1C

/* The offset of a plugin's data in a frame, or -1 when it is not registered. */
RwInt32 RwFrameGetPluginOffset(RwUInt32 pluginID) {
    return _rwPluginRegistryGetPluginOffset(&frameTKList, pluginID);
} // 0x80294E4C
