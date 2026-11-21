#ifndef GAME_CSCENEGRAPHCONTAINER_H
#define GAME_CSCENEGRAPHCONTAINER_H

#include <revolution/types.h>
#include <game/CBaseBehaviour.h>
#include <climax/Physics.h>
#include <climax/AttributeHandler.h>
#include <climax/EventHandler.h>
#include <game/unk_PhysicsManager.h>

class CCollidableSceneObject;

/*
 * A behaviour that keeps a fixed set of five physics bodies (entries) and a message id of its
 * own. Derived from CBaseBehaviour (vtable lbl_803E8F70, size 0x1DC). Member names invented;
 * the offsets without a use in the reconstructed functions stay as field_0x placeholders.
 */
class CSceneGraphContainer : public CBaseBehaviour {
public:
    /* One physics body with its state, 0x48 bytes. Name invented. */
    struct Entry {
        unk_RwpBody*    body;           // 0x0
        unk_RwMatrix    matrix;         // 0x4, the body's transform
        u32             field_0x44;     // 0x44
    };

    u32                 field_0x34;     // 0x34
    u32                 field_0x38;     // 0x38
    u32                 field_0x3C;     // 0x3C
    u16                 field_0x40;     // 0x40
    u8                  field_0x42[0x12];   // 0x42
    u32                 field_0x54;     // 0x54
    u32                 field_0x58;     // 0x58
    u32                 field_0x5C;     // 0x5C
    unk_801A92A8        id;             // 0x60, the message this container posts
    Entry               entries[5];     // 0x68
};

extern u32 lbl_805297D8;            // the attribute class id that fn_8009F4B4 reads

extern "C" {
void fn_8009F34C();
void fn_8009F4B4( CSceneGraphContainer* self, const unk_AttributePacket& packet );
void fn_8009F550();
void fn_8009F8F8( CSceneGraphContainer* self );
void fn_8009F954( CSceneGraphContainer* self );
void fn_8009FA78( CSceneGraphContainer* self );
void fn_8009FAE4();
void fn_8009FB8C( CCollidableSceneObject* self, unk_Msg& msg );
void fn_8009FBE4();
void fn_8009FCB0( u32* list, CCollidableSceneObject* object );
void fn_800A028C();
void fn_800A08A4();
void fn_800A0AAC();
void fn_800A11C4();
void fn_800A160C();
void fn_800A1D08();
void fn_800A22D8();
void fn_800A23AC();
void fn_800A2550();
void fn_800A260C();
void fn_800A2698();
void fn_800A26A0();
void fn_800A26A8();
void fn_800A26B0();
void fn_800A26B8();
void fn_800A26C0();
void fn_800A26C8();
void fn_800A26D0();
}

#endif
