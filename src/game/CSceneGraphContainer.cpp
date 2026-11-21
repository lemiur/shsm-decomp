// Unit name and boundary: class-name strings CSceneGraphContainer and CCollidableSceneObject in the DOL

#include <game/CSceneGraphContainer.h>
#include <game/CCollidableSceneObject.h>

u32 lbl_805297D8;

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .rodata 0x803D8900 size 0x10 lbl_803D8900
//   .data 0x803E8F40 size 0x16 lbl_803E8F40
//   .data 0x803E8F58 size 0x16 lbl_803E8F58
//   .data 0x803E8F70 size 0x60 lbl_803E8F70
//   .data 0x803E8FD0 size 0x150 lbl_803E8FD0
//   .bss 0x8046E860 size 0xC lbl_8046E860
//   .bss 0x8046E86C size 0xC lbl_8046E86C
//   .sbss 0x805297D8 size 0x4 lbl_805297D8
//   .sbss 0x805297DC size 0x4 lbl_805297DC
//   .sbss 0x805297E0 size 0x4 lbl_805297E0
//   .sbss 0x805297E4 size 0x4 lbl_805297E4
//   .sdata2 0x8052C308 size 0x4 lbl_8052C308
//   .sdata2 0x8052C30C size 0x4 lbl_8052C30C
//   .sdata2 0x8052C310 size 0x4 lbl_8052C310
//   .sdata2 0x8052C314 size 0x4 lbl_8052C314
//   .sdata2 0x8052C318 size 0x4 lbl_8052C318
//   .sdata2 0x8052C31C size 0x4 lbl_8052C31C
//   .sdata2 0x8052C320 size 0x4 lbl_8052C320
//   .sdata2 0x8052C324 size 0x4 lbl_8052C324
//   .sdata2 0x8052C328 size 0x8 lbl_8052C328
//   .sdata2 0x8052C330 size 0x8 lbl_8052C330
//   .sdata2 0x8052C338 size 0x4 lbl_8052C338
//   .sdata2 0x8052C33C size 0x4 lbl_8052C33C

/*
====================
fn_8009F34C
====================
*/
// 0x8009F34C
extern "C" void fn_8009F34C() {
    // TODO
}

/*
====================
fn_8009F4B4
====================
*/
// 0x8009F4B4
extern "C" void fn_8009F4B4( CSceneGraphContainer* self, const unk_AttributePacket& packet ) {
    self->CBaseBehaviour::fn_801A73A8( packet );
    for ( unk_AttributeCommandIterator it( packet, lbl_805297D8 ); !it.IsFinished(); ++it ) {
        if ( it->id == 0 ) {
            fn_801AA1C4( self->id, it->GetString(), "CSceneGraphContainer*" );
        }
    }
}

/*
====================
fn_8009F550
====================
*/
// 0x8009F550
extern "C" void fn_8009F550() {
    // TODO
}

/*
====================
fn_8009F8F8
====================
*/
// 0x8009F8F8
extern "C" void fn_8009F8F8( CSceneGraphContainer* self ) {
    PostMsg( self->id, self );
    fn_8009F954( self );
}

/*
====================
fn_8009F954
====================
*/
// 0x8009F954
extern "C" void fn_8009F954( CSceneGraphContainer* self ) {
    for ( int i = 0; i < 5; i++ ) {
        unk_RwpBodyDefinition* def = RwpBodyDefCreate();
        unk_RwpShape* shape = def->shape;
        RwpCapsuleInit( shape, 1.0f, 0.5f );
        fn_80310880( shape, 1.0f );
        fn_80310778( shape, 1.0f );
        RwpPartDefSetMassProperties( def, 0, 1.0f );
        unk_RwpBody* body = RwpBodyCreate( def );

        CSceneGraphContainer::Entry& entry = self->entries[i];
        entry.body = body;
        entry.matrix.right.x = 1.0f;
        entry.matrix.right.y = 0.0f;
        entry.matrix.right.z = 0.0f;
        entry.matrix.flags |= 0x20003;
        entry.matrix.up.x = 0.0f;
        entry.matrix.up.y = 1.0f;
        entry.matrix.up.z = 0.0f;
        entry.matrix.at.x = 0.0f;
        entry.matrix.at.y = 0.0f;
        entry.matrix.at.z = 1.0f;
        entry.matrix.pos.x = 0.0f;
        entry.matrix.pos.y = 0.0f;
        entry.matrix.pos.z = 0.0f;
        body->parts->matrix = &entry.matrix;
        fn_8032110C( body, 0 );
        RwpBodyDefDestroy( def );
    }
}

/*
====================
fn_8009FA78
====================
*/
// Open: retail keeps the loop counter in r30 and the entry pointer in r29; this build swaps them (register/schedule).
// 0x8009FA78
extern "C" void fn_8009FA78( CSceneGraphContainer* self ) {
    for ( int i = 0; i < 5; i++ ) {
        CSceneGraphContainer::Entry& entry = self->entries[i];
        lbl_80529170->fn_8000F71C( entry.body );
        fn_802FA9D4( entry.body );
        entry.body = NULL;
        entry.field_0x44 = 0;
    }
}

/*
====================
fn_8009FAE4
====================
*/
// 0x8009FAE4
extern "C" void fn_8009FAE4() {
    // TODO
}

/*
====================
fn_8009FB8C
====================
*/
// Open: the compiled body drops the message test and the call (wall: call set, control flow); not closed.
// 0x8009FB8C
extern "C" void fn_8009FB8C( CCollidableSceneObject* self, unk_Msg& msg ) {
    self->CSceneObject::fn_8009CB1C( msg );
    if ( msg.record == self->id.record ) {
        CSceneGraphContainer* container = ( CSceneGraphContainer* )msg.data;
        fn_8009FCB0( &container->field_0x34, self );
    }
}

/*
====================
fn_8009FBE4
====================
*/
// 0x8009FBE4
extern "C" void fn_8009FBE4() {
    // TODO
}

/*
====================
fn_8009FCB0
====================
*/
// 0x8009FCB0
extern "C" void fn_8009FCB0( u32* list, CCollidableSceneObject* object ) {
    // TODO
}

/*
====================
fn_800A028C
====================
*/
// 0x800A028C
extern "C" void fn_800A028C() {
    // TODO
}

/*
====================
fn_800A08A4
====================
*/
// 0x800A08A4
extern "C" void fn_800A08A4() {
    // TODO
}

/*
====================
fn_800A0AAC
====================
*/
// 0x800A0AAC
extern "C" void fn_800A0AAC() {
    // TODO
}

/*
====================
fn_800A11C4
====================
*/
// 0x800A11C4
extern "C" void fn_800A11C4() {
    // TODO
}

/*
====================
fn_800A160C
====================
*/
// 0x800A160C
extern "C" void fn_800A160C() {
    // TODO
}

/*
====================
fn_800A1D08
====================
*/
// 0x800A1D08
extern "C" void fn_800A1D08() {
    // TODO
}

/*
====================
fn_800A22D8
====================
*/
// 0x800A22D8
extern "C" void fn_800A22D8() {
    // TODO
}

/*
====================
fn_800A23AC
====================
*/
// 0x800A23AC
extern "C" void fn_800A23AC() {
    // TODO
}

/*
====================
fn_800A2550

Address taken by the static initialiser: probably an inline factory that belongs in the header.
====================
*/
// 0x800A2550
extern "C" void fn_800A2550() {
    // TODO
}

/*
====================
fn_800A260C

Address taken by the static initialiser: probably an inline factory that belongs in the header.
====================
*/
// 0x800A260C
extern "C" void fn_800A260C() {
    // TODO
}

/*
====================
fn_800A2698
====================
*/
// 0x800A2698
extern "C" void fn_800A2698() {
    // TODO
}

/*
====================
fn_800A26A0
====================
*/
// 0x800A26A0
extern "C" void fn_800A26A0() {
    // TODO
}

/*
====================
fn_800A26A8
====================
*/
// 0x800A26A8
extern "C" void fn_800A26A8() {
    // TODO
}

/*
====================
fn_800A26B0
====================
*/
// 0x800A26B0
extern "C" void fn_800A26B0() {
    // TODO
}

/*
====================
fn_800A26B8
====================
*/
// 0x800A26B8
extern "C" void fn_800A26B8() {
    // TODO
}

/*
====================
fn_800A26C0
====================
*/
// 0x800A26C0
extern "C" void fn_800A26C0() {
    // TODO
}

/*
====================
fn_800A26C8
====================
*/
// 0x800A26C8
extern "C" void fn_800A26C8() {
    // TODO
}

/*
====================
fn_800A26D0
====================
*/
// 0x800A26D0
extern "C" void fn_800A26D0() {
    // TODO
}

// Compiler-generated, no source of their own: fn_800A24A8 (0x800A24A8).
