#ifndef GAME_CSPLINEHELPER_H
#define GAME_CSPLINEHELPER_H

#include <climax/AttributeHandler.h>
#include <climax/EventHandler.h>
#include <climax/Memory.h>
#include <climax/unk_801A92A8.h>
#include <game/CBaseBehaviour.h>

class CSplineHelper;

/*
 * The table lbl_803F66B8 (0x28 bytes: two zero words, seven slots and a null slot), whose owning
 * unit is fn_8007566C's. Name invented.
 */
struct unk_SplineOps;
extern unk_SplineOps lbl_803F66B8;

/* The chunk record fn_801ABE14 finds for a command name. Name invented. */
struct unk_SplineRecord {
    u32     field_0x0[5];   // 0x0..0x13
    u32     value;          // 0x14
};

/* The payload of the message the helper posts: the helper and whether it is still alive. Name invented. */
struct unk_SplinePayload {
    CSplineHelper*  owner;  // 0x0
    bool            alive;  // 0x4
};

/*
 * A behaviour that holds a spline. Registered as "CSplineHelper" (lbl_80529838 is the hash of that
 * name, lbl_8052983C its registry entry), vtable __vt__12CSplineHelper, size 0xDC. Its factory
 * fn_800A8024 inlines the constructor. Member names invented.
 */
class CSplineHelper : public CBaseBehaviour {
public:
    CSplineHelper() {
        field_0x44 = 0;
        field_0x4C = 2;
        field_0x54 = 0;
        field_0x5C = 0;
        field_0x58 = 0;
        field_0x60 = 0;
        field_0x48 = &lbl_803F66B8;
        field_0x78 = 0.0f;
        field_0x7C = 0.0f;
        field_0x80 = 0.0f;
        field_0x84 = 2;
        field_0x88 = 0;
        field_0x8C = 0;
        field_0xB0 = 2;
        field_0xB4 = 0;
        field_0xB8 = 0;
        field_0x64 = 0;
    }
    virtual void    fn_8009CB1C( unk_Msg& msg );                        // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );    // override
    virtual         ~CSplineHelper();                                    // override

    static CAttributeHandler* fn_800A8024( const unk_AttributePacket& /* packet */ ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CSplineHelper();
    }

    unk_801A92A8    listenMsg;      // 0x34, linked by the handler for command 0; its messages reach fn_8009CB1C
    unk_801A92A8    postMsg;        // 0x3C, registered for command 1; posted when listenMsg arrives
    u32             field_0x44;     // 0x44, the spline value of the rwID_SPLINE record
    unk_SplineOps*  field_0x48;     // 0x48, the table of fn_8013CEF8
    u32             field_0x4C;     // 0x4C
    u32             field_0x50;     // 0x50
    u32             field_0x54;     // 0x54
    u32             field_0x58;     // 0x58
    u32             field_0x5C;     // 0x5C
    u32             field_0x60;     // 0x60
    u32             field_0x64;     // 0x64, the spline value of the rwID_SPLINE record
    u32             field_0x68[4];  // 0x68..0x77
    float           field_0x78;     // 0x78
    float           field_0x7C;     // 0x7C
    float           field_0x80;     // 0x80
    u32             field_0x84;     // 0x84
    u32             field_0x88;     // 0x88
    u32             field_0x8C;     // 0x8C
    u32             field_0x90[8];  // 0x90..0xAF
    u32             field_0xB0;     // 0xB0
    u32             field_0xB4;     // 0xB4
    u32             field_0xB8;     // 0xB8
    u32             field_0xBC[8];  // 0xBC..0xDB
};

extern u32 lbl_80529838;            // class-name hash of "CSplineHelper"
extern unk_801A8F6C lbl_8052983C;   // type-registry entry of CSplineHelper

#endif
