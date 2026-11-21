#ifndef GAME_UNK_80009628_H
#define GAME_UNK_80009628_H

#include <revolution/types.h>
#include <climax/Frame.h>
#include <revolution/gx/GXStruct.h>

// One record of the depth-query table lbl_804576A0 (eight records). Names of the fields are
// placeholders: only the accesses in this unit's functions are known.
struct unk_DepthQuery {
    unk_RwMatrix    matrix;             // 0x00: copied from the engine state at 0x20
    u32             sampleIndex;        // 0x40: index of the 16-bit sample in the readback record
    s32             screenX;            // 0x44
    s32             screenY;            // 0x48
    float           nearPlane;          // 0x4C: from the engine state at 0x80
    float           farPlane;           // 0x50: from the engine state at 0x84
    s32             frame;              // 0x54
    void*           owner;              // 0x58: compared with the caller's key
    u8              active;             // 0x5C
    u8              field_0x5D;         // 0x5D
    u16             token;              // 0x5E: display-list token the sample waits on
    u8              field_0x60[4];      // 0x60
};

// The readback record of one query: a 32-byte block the GX texture copy fills.
struct unk_DepthReadback {
    u16             samples[16];
};

extern unk_DepthQuery   lbl_804576A0[8];
extern unk_DepthReadback lbl_804575A0[8];
extern s32              lbl_80529154;   // count of queries issued (the frame number of the newest one)
extern s32              lbl_80529150;   // ring index of the next record

// The render mode in use (defined in rwsdk/src/driver/wii/dlsprite.c): its copy filter is what the readback restores.
extern GXRenderModeObj* lbl_8052B2A4;

extern "C" {
void fn_80009628(void* owner);
BOOL fn_80009AB8(void* owner, unk_RwV3d* out, BOOL useNewest);
void* fn_80009CCC(u32 size);
BOOL fn_80009CD4(void* block);

// Block allocator and release routines in the unstarted auto unit at 0x8018DBE8 (names unknown).
void* fn_8018DDD8(u32 size, u32 flags);
void fn_8018DD78(void* block);
}

#endif
