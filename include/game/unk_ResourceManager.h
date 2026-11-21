#ifndef GAME_UNK_RESOURCEMANAGER_H
#define GAME_UNK_RESOURCEMANAGER_H

#include <revolution/types.h>

/*
 * The resource manager behind lbl_8052A5C0 (unit at 0x801B0120 not yet reconstructed). Name
 * invented. Only the first word and the flag at 0x10 are modelled; the rest of the layout is not
 * reconstructed.
 */
struct unk_ResourceManager {
    u32     field_0x0;      // 0x0, read by fn_80015F8C
    u8      gap_0x4[0xC];   // not reconstructed
    u8      field_0x10;     // read by fn_80029444; meaning not yet known
};

extern unk_ResourceManager* lbl_8052A5C0;

/*
 * Reads size bytes from stream into buffer through the manager, returns the byte count read.
 * Unit at 0x801B10AC not yet reconstructed; name and signature as the caller uses them.
 */
extern "C" {
u32 fn_801B10AC( unk_ResourceManager* manager, void* stream, void* buffer, u32 size );
// defined at 0x801B0F5C, unit not yet reconstructed; the manager argument is not read there
u32 fn_801B0F5C( unk_ResourceManager* manager, void* obj );
}
// defined in 0x801B0A88, unit not yet reconstructed; the third argument is not read there
extern "C" void fn_801B0A88( unk_ResourceManager* manager, void* handle, void* unused, int flags );

#endif
