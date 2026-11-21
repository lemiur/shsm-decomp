// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/Effects.h>
#include <rwsdk/baframe.h>
#include <rwsdk/plugin/hanim/rphanim.h>

/* A node that is re-linked to a new owner. Name invented; members unknown. */
struct unk_EffectNode {
    u32     field_0x0;
    void*   field_0x4;
    char    field_0x8[0x5C];
    void*   field_0x64;
};

// The list helpers the node is moved between; their units are not reconstructed.
extern "C" void fn_80027738( void* owner, void* node );
extern "C" void fn_80027740( void* owner, void* node );

struct unk_RpClump;
// The clump's hierarchy; the declaration in climax/Utils.h types it as the placeholder unk_RpHAnimHierarchy.
RpHAnimHierarchy* fn_801B6AD8( unk_RpClump* clump );

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803E83C8 size 0x30 lbl_803E83C8
//   .data 0x803E83F8 size 0x10 lbl_803E83F8
//   .data 0x803E8408 size 0x9 lbl_803E8408
//   .data 0x803E8414 size 0x78 lbl_803E8414
//   .data 0x803E848C size 0x9 lbl_803E848C
//   .data 0x803E8498 size 0x9 lbl_803E8498
//   .data 0x803E84A4 size 0x9 lbl_803E84A4
//   .data 0x803E84B0 size 0x14 lbl_803E84B0
//   .data 0x803E84C8 size 0x10 lbl_803E84C8
//   .sdata 0x80527270 size 0x4 lbl_80527270
//   .sdata 0x80527274 size 0x4 lbl_80527274
//   .sdata 0x80527278 size 0x4 lbl_80527278
//   .sdata 0x8052727C size 0x4 lbl_8052727C
//   .sdata2 0x8052C0B0 size 0x4 lbl_8052C0B0
//   .sdata2 0x8052C0B4 size 0x4 lbl_8052C0B4
//   .sdata2 0x8052C0B8 size 0x4 lbl_8052C0B8
//   .sdata2 0x8052C0BC size 0x4 lbl_8052C0BC

/*
====================
Effects::CalculateEase
====================
*/
// 0x80089F10
float Effects::CalculateEase( float t, float easeIn, float easeOut ) {
    float span = easeIn + easeOut;
    if ( span == 0.0f ) {
        return t;
    }
    if ( span > 1.0f ) {
        easeIn /= span;
        easeOut /= span;
    }
    float peak = 1.0f / ( 2.0f - easeIn - easeOut );
    if ( t < easeIn ) {
        return t * ( t * ( peak / easeIn ) );
    }
    if ( t < 1.0f - easeOut ) {
        return peak * ( 2.0f * t - easeIn );
    }
    return 1.0f - ( 1.0f - t ) * ( ( 1.0f - t ) * ( peak / easeOut ) );
}

/*
====================
fn_80089F94
====================
*/
// 0x80089F94
extern "C" void fn_80089F94() {
    // TODO: open: deleting destructor of the class built by fn_8008A00C (vtable lbl_803E84C8);
    // class layout and the shared id objects lbl_805296D0 and lbl_8052A790 are not declared yet.
}

/*
====================
fn_8008A00C
====================
*/
// 0x8008A00C
extern "C" void fn_8008A00C() {
    // TODO: open: factory that news a 0x6B24-byte object of the class with vtable lbl_803E84C8;
    // the class layout is not recovered.
}

/*
====================
fn_8008A188
====================
*/
// 0x8008A188
extern "C" void fn_8008A188( void*, unk_Msg& ) {
    // TODO: open: message handler slot. Its body compares msg.record with lbl_8052A790, reads the
    // player record's field_0x20 and field_0x28, and tail-calls fn_8008C3F0 with the scaled frame
    // time. It needs unk_Msg (climax/EventHandler.h), which conflicts with this unit's RenderWare
    // headers (climax/Frame.h RwMatrixUpdate against rwsdk bamatrix.h).
}

/*
====================
fn_8008A1C0
====================
*/
// 0x8008A1C0
extern "C" void fn_8008A1C0() {
    // TODO: open: loop over the effect entries at offset 0x3c0c (stride 0x6C) calling a
    // per-entry callback; the entry struct is not recovered.
}

/*
====================
fn_8008A288
====================
*/
// 0x8008A288
extern "C" void fn_8008A288() {
    // TODO: open: effect-entry lookup in the 0x3c0c table; the entry struct is not recovered.
}

/*
====================
fn_8008A378
====================
*/
// 0x8008A378
extern "C" void fn_8008A378() {
    // TODO: open: search loop over the 0x100c entry table comparing key fields; the entry
    // struct is not recovered.
}

/*
====================
fn_8008A450
====================
*/
// 0x8008A450
extern "C" void fn_8008A450() {
    // TODO: open: large effect update routine (0x4E4 bytes); its structure is not recovered.
}

/*
====================
fn_8008A934
====================
*/
// 0x8008A934
extern "C" void fn_8008A934() {
    // TODO
}

/*
====================
fn_8008AC70
====================
*/
// 0x8008AC70
extern "C" void fn_8008AC70() {
    // TODO
}

/*
====================
unk_EffectBank::fn_8008AF18
====================
*/
// 0x8008AF18
unk_Effect* unk_EffectBank::fn_8008AF18( const char* ) {
    // TODO
    return NULL;
}

/*
====================
fn_8008AFB0
====================
*/
// 0x8008AFB0
extern "C" void fn_8008AFB0() {
    // TODO
}

/*
====================
fn_8008B050
====================
*/
// 0x8008B050
extern "C" void fn_8008B050() {
    // TODO
}

/*
====================
fn_8008B0AC
====================
*/
// 0x8008B0AC
extern "C" void fn_8008B0AC() {
    // TODO
}

/*
====================
Effects::GetBaseMatrix
====================
*/
// 0x8008B110
void Effects::GetBaseMatrix( RwMatrixTag& matrix, void* object ) {
    RwFrame* frame = (RwFrame*)( (RwObject*)object )->parent;
    matrix = frame->modelling;
}

/*
====================
Effects::GetMatrixByTag
====================
*/
// 0x8008B198
void Effects::GetMatrixByTag( unsigned int tag, RwMatrixTag& matrix, void* object ) {
    RpHAnimHierarchy* hierarchy = fn_801B6AD8( (unk_RpClump*)object );
    RwUInt32 index = RpHAnimIDGetIndex( hierarchy, tag );
    if ( index >= hierarchy->numNodes ) {
        GetBaseMatrix( matrix, object );
    } else {
        matrix = RpHAnimHierarchyGetMatrixArray( hierarchy )[index];
    }
}

/*
====================
fn_8008B31C
====================
*/
// 0x8008B31C
extern "C" void fn_8008B31C() {
    // TODO
}

/*
====================
fn_8008B348
====================
*/
// 0x8008B348
extern "C" void fn_8008B348() {
    // TODO
}

/*
====================
fn_8008B9CC
====================
*/
// 0x8008B9CC
extern "C" void fn_8008B9CC( void*, unk_EffectNode* node, void* owner ) {
    if ( node->field_0x0 != 0 ) {
        void* link = node->field_0x4;
        if ( link != 0 ) {
            fn_80027740( node->field_0x64, link );
            node->field_0x64 = owner;
            fn_80027738( owner, link );
        }
    }
}

/*
====================
fn_8008BA3C
====================
*/
// 0x8008BA3C
extern "C" void fn_8008BA3C() {
    // TODO
}

/*
====================
fn_8008BC70
====================
*/
// 0x8008BC70
extern "C" void fn_8008BC70() {
    // TODO
}

/*
====================
fn_8008BFC8
====================
*/
// 0x8008BFC8
extern "C" void fn_8008BFC8() {
    // TODO
}
