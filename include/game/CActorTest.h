#ifndef GAME_CACTORTEST_H
#define GAME_CACTORTEST_H

#include <revolution/types.h>
#include <climax/Frame.h>
#include <climax/Memory.h>
#include <game/unk_8001777C.h>

unk_RwV3d fn_8001B200( unk_8001B000* obj );

/*
 * Test actor behaviour derived from unk_8001777C: cycles through animations on frame updates
 * when its state changes, optionally queries a target object's frame matrix and translates its
 * own frame using fn_8001B200. vtable __vt__10CActorTest, size 0x1E8.
 * Member and command enum names invented.
 */
class CActorTest : public unk_8001777C {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetField0x1D4,      // integer: nonzero sets field_0x1D4
        CMD_SetEvent1D8,        // text: query-response message name
        CMD_SetEvent1E0         // text: target query message name
    };

                    CActorTest( const unk_AttributePacket& packet );
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~CActorTest();  // override
    virtual void    fn_801A7A88();  // override

    static CAttributeHandler* fn_8003C384( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CActorTest( packet );
    }

    char            field_0x1BC[0x10];  // 0x1BC, "Actor%02d" instance name
    s32             field_0x1CC;        // 0x1CC
    s32             field_0x1D0;        // 0x1D0
    bool            field_0x1D4;        // 0x1D4
    bool            field_0x1D5;        // 0x1D5
    unk_801A92A8    field_0x1D8;        // 0x1D8
    unk_801A92A8    field_0x1E0;        // 0x1E0
};

extern s32          lbl_80529320;
extern u32          lbl_80529324;   // class-name hash of "CActorTest"
extern unk_801A8F6C lbl_80529328;   // type-registry entry of CActorTest
extern s32          lbl_8052932C;   // instance counter for "Actor%02d"

#endif
