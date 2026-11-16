// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Processor/ppc/Export/targsupp.s
// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/src/MetroTRK/targsupp.s
// (an assembly file in both references; written as asm functions in C like the project's other ORIGINAL-ASM
// bodies)

#include "TRK/targsupp.h"

// ORIGINAL-ASM: traps into the debugger, which services the request (twui); assembly in tp's and xenoblade's targsupp.s
/* Read or write request on an open host file. */
asm u32 TRKAccessFile(u32 command, u32 handle, u32* count, u8* buffer) {
    nofralloc
    twi 31, r0, 0
    blr
} // 0x80190BEC

// ORIGINAL-ASM: traps into the debugger, which services the request (twui); assembly in tp's and xenoblade's targsupp.s
/* Open request for a host file. */
asm u32 TRKOpenFile(u32 command, u32 path, u32* handle, u8* name) {
    nofralloc
    twi 31, r0, 0
    blr
} // 0x80190BF4

// ORIGINAL-ASM: traps into the debugger, which services the request (twui); assembly in tp's and xenoblade's targsupp.s
/* Close request for a host file. */
asm u32 TRKCloseFile(u32 command, u32 handle) {
    nofralloc
    twi 31, r0, 0
    blr
} // 0x80190BFC

// ORIGINAL-ASM: traps into the debugger, which services the request (twui); assembly in tp's and xenoblade's targsupp.s
/* Seek request on a host file. */
asm u32 TRKPositionFile(u32 command, u32 handle, u32* position, u8* seekMode) {
    nofralloc
    twi 31, r0, 0
    blr
} // 0x80190C04
