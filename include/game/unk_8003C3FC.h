#ifndef GAME_UNK_8003C3FC_H
#define GAME_UNK_8003C3FC_H

#include <revolution/types.h>
#include <game/unk_8000F80C.h>
#include <game/unk_8003C454.h>

/*
 * The source a unk_ActorSlot names. Name invented: its class is not reconstructed. It derives
 * from unk_8000F80C (the GetName of fn_80043658 reads through the slot's first word), and the
 * field at 0x38 is a word the slot callbacks test for zero.
 */
class unk_ActorSlotSource : public unk_8000F80C {
public:
    u8              field_0x18[0x20];
    s32             field_0x38;
};

/*
 * The record at field_0x220 of unk_8003C454. Name invented: its constructor is not reconstructed.
 * The callbacks below set its word at 0x20 to 1.
 */
class unk_ActorSlot {
public:
    unk_ActorSlotSource* source;    // 0x0
    u8              field_0x4[0x1C];
    s32             field_0x20;     // 0x20
    u8              field_0x24[0x4];
    unk_EventDescriptor* field_0x28;    // 0x28, the event descriptor whose count and entries bound the list (fn_8003CCC4)
};

/*
 * Callbacks registered with fn_8004B938 and fn_8004B944 (the context goes in the first argument,
 * which they ignore; the second argument is the actor they act on, typed unk_8003C454 from the
 * field at 0x220 they read). Names invented.
 */
extern "C" {
void fn_8003C3FC( void* context, unk_8003C454* who );
void fn_8003C424( void* context, unk_8003C454* who );
void fn_8003C428( void* context, unk_8003C454* who );
void fn_8003C450( void* context, unk_8003C454* who );
}

#endif
