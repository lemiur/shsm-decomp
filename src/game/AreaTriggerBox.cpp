// unit name unconfirmed: the class names are from the registration strings and the log strings
// "AreaTriggerBox[%s] Sends %s (Enter Event)" and "(Exit Event)"; the file name is invented.
// The inline factory fn_800D1890 and the two thunks follow the static initialiser in .text.

#include <game/AreaTriggerBox.h>
#include <game/Log.h>
#include <game/SaveData.h>

const char* lbl_80527578 = "SaveData";

u32 lbl_80529C30 = fn_801A7A84( "AreaTriggerBox" );
unk_801A8F6C lbl_80529C34( "AreaTriggerBox", AreaTriggerBox::fn_800D1890, sizeof( AreaTriggerBox ) );
u32 lbl_80529C38 = fn_801A7A84( "AreaTriggerBoxLogic" );

/*
====================
AreaTriggerBox::~AreaTriggerBox
====================
*/
// 0x800D1268
AreaTriggerBox::~AreaTriggerBox() {
    delete saveDataA;
    delete saveDataB;
    delete field_0xE8;
    fn_801A9940( enterMsg );
    fn_801A9940( exitMsg );
}

/*
====================
AreaTriggerBox::fn_801A73A8

Takes the enter and exit messages, the save-data value and the third key from the attribute
packet.
====================
*/
// 0x800D1328
void AreaTriggerBox::fn_801A73A8( const unk_AttributePacket& packet ) {
    AreaTriggerBoxLogic::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529C30 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetEnterMsg:
            fn_801AA1C4( enterMsg, it->GetString(), 0 );
            break;
        case CMD_SetExitMsg:
            fn_801AA1C4( exitMsg, it->GetString(), 0 );
            break;
        case CMD_SetSaveDataA:
            saveDataA = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetSaveDataB:
            saveDataB = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField0xE8:
            field_0xE8 = fn_801B67FC( it->GetString(), 0 );
            break;
        }
        ++it;
    }
}

/*
====================
AreaTriggerBox::fn_800D1434

Sends the enter message while the save-data value is set.
====================
*/
// 0x800D1434
void AreaTriggerBox::fn_800D1434( void* data ) {
    if ( fn_80206348( saveDataA, saveDataB, lbl_80527578 ) && CheckKey() ) {
        fn_80028D0C( 4, "AreaTriggerBox[%s] Sends %s (Enter Event)", NULL,
                     enterMsg.record != NULL ? enterMsg.record->name : NULL );
        PostMsg( enterMsg, data );
    }
}

/*
====================
AreaTriggerBox::fn_800D14F0

Sends the exit message while the save-data value is set.
====================
*/
// 0x800D14F0
void AreaTriggerBox::fn_800D14F0( void* data ) {
    if ( fn_80206348( saveDataA, saveDataB, lbl_80527578 ) && CheckKey() ) {
        fn_80028D0C( 4, "AreaTriggerBox[%s] Sends %s (Exit Event)", NULL,
                     exitMsg.record != NULL ? exitMsg.record->name : NULL );
        PostMsg( exitMsg, data );
    }
}

/*
====================
AreaTriggerBoxLogic::~AreaTriggerBoxLogic
====================
*/
// 0x800D15AC
AreaTriggerBoxLogic::~AreaTriggerBoxLogic() {}

/*
====================
AreaTriggerBoxLogic::fn_801A73A8
====================
*/
// 0x800D1604
void AreaTriggerBoxLogic::fn_801A73A8( const unk_AttributePacket& packet ) {
    AreaTriggerBase::fn_801A73A8( packet );
}

/*
====================
AreaTriggerBoxLogic::fn_800D1608

Whether the point lies inside the box.
====================
*/
// 0x800D1608
bool AreaTriggerBoxLogic::fn_800D1608( const unk_Vec3* point ) {
    unk_Vec3 local;
    RwV3dTransformPoints( &local, point, 1, &inverse );
    if ( local.x < 0.5f && local.x > -0.5f &&
         local.y < 0.5f && local.y > -0.5f &&
         local.z < 0.5f && local.z > -0.5f ) {
        return true;
    }
    return false;
}

/*
====================
Dot

The dot product of a point and a plane normal. Name invented.
====================
*/
static inline float Dot( const unk_Vec3* a, const unk_RwV3d* b ) {
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

/*
====================
InsidePlane

Whether a point lies on the inner side of a face. Returns an int truth value, as RenderWare's
RwBool. Name invented.
====================
*/
static inline int InsidePlane( const unk_RwPlane* plane, const unk_Vec3* point ) {
    return Dot( point, &plane->normal ) + plane->distance <= 0.0f;
}

/*
====================
AreaTriggerBoxLogic::fn_800D1688

Whether the segment can touch the box: false once both its ends lie outside one face.
====================
*/
// 0x800D1688
bool AreaTriggerBoxLogic::fn_800D1688( const unk_Vec3* segment ) {
    unk_RwPlane faces[NUM_FACES] = {
        { {  1.0f,  0.0f,  0.0f }, -0.5f },
        { { -1.0f,  0.0f,  0.0f }, -0.5f },
        { {  0.0f,  1.0f,  0.0f }, -0.5f },
        { {  0.0f, -1.0f,  0.0f }, -0.5f },
        { {  0.0f,  0.0f,  1.0f }, -0.5f },
        { {  0.0f,  0.0f, -1.0f }, -0.5f }
    };
    unk_Vec3 local[2];
    RwV3dTransformPoints( local, segment, 2, &inverse );
    for ( int i = 0; i < NUM_FACES; i++ ) {
        int inside0 = InsidePlane( &faces[i], &local[0] );
        int inside1 = InsidePlane( &faces[i], &local[1] );
        if ( !inside0 && !inside1 ) {
            return false;
        }
    }
    return true;
}

/*
====================
AreaTriggerBoxLogic::fn_800D1818
====================
*/
// 0x800D1818
void AreaTriggerBoxLogic::fn_800D1818() {}
