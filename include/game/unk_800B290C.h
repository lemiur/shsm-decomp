#ifndef GAME_UNK_800B290C_H
#define GAME_UNK_800B290C_H

#include <revolution/types.h>

/*
 * Cursor over a buffer that is read in 16 KiB blocks. The class name is invented: its constructor,
 * which stores the vtable, is not reconstructed here.
 * Member names invented. Only the members the functions in this unit touch are named.
 */
struct unk_800B2E58 {
    u8      field_0x0[0x14];    // vptr and the members before the cursor, not identified here
    u32     cursor;             // 0x14: the address the last seek was given
    u32     blockOffset;        // 0x18: the cursor's offset from the buffer base
    u8      field_0x1C[0x4];
    u32*    bufferBase;         // 0x20: points at the buffer's start address
    u8      field_0x24[0xC];
    u32     blockCount;         // 0x30: 16 KiB blocks the window holds
};

extern u32 lbl_805273D4;        // .sdata word whose address fn_800B290C returns

extern "C" {
u32* fn_800B290C();
int fn_800B2914();
int fn_800B291C();
u32 fn_800B2934( unk_800B2E58* self, u32 address );
u32 fn_800B293C( unk_800B2E58* self, u32 address );
u32 fn_800B2940( unk_800B2E58* self, u32 address );

/* Not in this unit: */
u32 fn_800B2ED4( unk_800B2E58* self, u32 address );
s32 fn_800B3144( unk_800B2E58* self, s32 blocks );
u32 fn_800B2FE8( unk_800B2E58* self, u32 address );
}

#endif
