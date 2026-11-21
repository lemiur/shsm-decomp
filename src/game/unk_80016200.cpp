// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80016200.h>
#include <climax/Heap.h>

/*
====================
unk_80016200::unk_80016200
====================
*/
// 0x80016200
unk_80016200::unk_80016200() {
    // 0x10000-byte block (64 KiB) from heap 2 with the factories' flags; its use is not recovered.
    field_0x34 = new ( GAME_HEAP, GAME_HEAP_FLAGS ) u8[ 0x10000 ];
}
