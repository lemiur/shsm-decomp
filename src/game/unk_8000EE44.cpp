// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8000EE44.h>
#include <game/unk_PhysicsManager.h>

/*
====================
fn_8000EE44
====================
*/
// 0x8000EE44
extern "C" void fn_8000EE44() {
    lbl_80529170 = new (GAME_HEAP, GAME_HEAP_FLAGS) unk_PhysicsManager;
}
