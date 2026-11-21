#ifndef GAME_MSGTRIGGER_H
#define GAME_MSGTRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

/*
 * The behaviour registered as "MsgTrigger" (vtable lbl_803E9DC8, size 0x34). It adds no members
 * and overrides no slot of CBaseBehaviour: its vtable is the base's, with the secondary base's
 * destructor thunk.
 */
class MsgTrigger : public CBaseBehaviour {
public:
    virtual         ~MsgTrigger();
};

/* Factory of the registry entry: allocates a MsgTrigger on the game heap. The packet is unused. */
extern "C" inline CAttributeHandler* fn_800AB210( const unk_AttributePacket& packet ) {
    return new( GAME_HEAP, GAME_HEAP_FLAGS ) MsgTrigger;
}

extern u32 lbl_80529858;            // class-name hash of "MsgTrigger" (fn_801A7A84)
extern unk_801A8F6C lbl_8052985C;   // type-registry entry of MsgTrigger

#endif
