#ifndef GAME_UNK_80016644_H
#define GAME_UNK_80016644_H

#include <revolution/types.h>
#include <game/unk_80016634.h>

// The dispatcher at 0x80016644 is slot 13 of the vtable lbl_803E07F0 (constructor fn_80016200,
// unit not yet reconstructed). Its owning class and the message type at msg+0x8 are not yet
// recovered, so the self and message parameters are untyped.


extern "C" {
void fn_80016644( void* self, void* msg, s32 value );

// Handlers of the states 0 and 2, not yet reconstructed; their signatures follow their caller.
void fn_80016678( void* self, void* msg, s32 value );
void fn_800168F8( void* self, void* msg, s32 value );
}

#endif
