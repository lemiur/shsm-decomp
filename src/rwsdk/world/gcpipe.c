// Unit name and boundary: Gormiti link map (RPworld.a gcpipe)
// Locals and names: Gormiti DWARF
/*
 * gcpipe.c: the GameCube instanced pipeline's resource entries. A vertex buffer's header sits
 * right after its resource entry; before the entry is reused the pipeline waits for the GX draw
 * sync token of the last draw that read it.
 */
#include <revolution/gx/GXManage.h>
#include <rwsdk/world/gcpipe.h>

/* Waits until the GX pipe has drawn from the resource entry's vertex buffer, asking for a draw sync if it hasn't one. */
void _rxGCResEntryWaitDone(RwResEntry* resEntry) {
    RxGameCubeVertexBuffer* vbHeader = (RxGameCubeVertexBuffer*)(resEntry + 1);

    if (vbHeader->token == _RwDlTokenCurrent) {
        GXSetDrawSync(_RwDlTokenCurrent);
        _RwDlTokenCurrent = (_RwDlTokenCurrent + 1) % rwDLTOKENMODULUS;
    }

    while (!_rwDlTokenQueryDone(vbHeader->token)) {
    }
} // 0x802DC674
