// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80039E40.h>
#include <climax/Audio/EventSource.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803E2548 size 0x20 lbl_803E2548
//   .sdata2 0x8052B950 size 0x4 lbl_8052B950
//   .sdata2 0x8052B954 size 0x4 lbl_8052B954
//   .sdata2 0x8052B958 size 0x8 lbl_8052B958

/*
====================
unk_80039E40::unk_80039E40
====================
*/
// 0x80039E40
unk_80039E40::unk_80039E40() {
    // TODO: wall (referent): the members at 0x0, 0x4 (STLList) and 0x18 (RwMatrix) are untyped, and fn_800350E0 has no signature yet.
}

/*
====================
unk_80039E40::~unk_80039E40
====================
*/
// 0x80039EF8
unk_80039E40::~unk_80039E40() {
    // TODO: wall (layout): 128 instructions and four loops over the untyped list members; not analysed in this pass.
}

/*
====================
fn_8003A0F8
====================
*/
// 0x8003A0F8
extern "C" void fn_8003A0F8() {
    // TODO: wall (size): 175 instructions, six loops, new and delete; not analysed in this pass.
}

/*
====================
fn_8003A3B4
====================
*/
// The first argument is passed in r3 and never read by retail; the second is the source tested.
// 0x8003A3B4
extern "C" bool fn_8003A3B4( void*, Audio::EventSource* source ) {
    if ( !source->IsPlayingOrInPlayingState() ) {
        if ( !source->IsPaused() ) {
            return false;
        }
    }
    return true;
}

/*
====================
fn_8003A420
====================
*/
// 0x8003A420
extern "C" void fn_8003A420() {
    // TODO: wall (types): 152 instructions and seven calls; the element type of the traversal in fn_8003A680 is untyped.
}

/*
====================
fn_8003A680
====================
*/
// 0x8003A680
extern "C" void fn_8003A680() {
    // TODO: wall (types): list traversal over this+0xC with a nested list at element+0x1C; element types and the six-argument contract are untyped.
}

/*
====================
fn_8003A770
====================
*/
// 0x8003A770
extern "C" void fn_8003A770() {
    // TODO: wall (referent): the object at this+0x0 has no type with a matrix at 0x1C0 for RwMatrixInvert.
}

/*
====================
fn_8003A7BC
====================
*/
// 0x8003A7BC
extern "C" void fn_8003A7BC() {
    // TODO: wall (referent): the object at this+0x0 has no type with matrices at 0x1C0 and 0x180 for the RwMatrixInvert pair.
}

/*
====================
Audio::CAudioRelay::PlaySound
====================
*/
// 0x8003A898
// TODO: signature not expanded or needs a complete type: PlaySound__Q25Audio11CAudioRelayFPCcPC5RwV3dQ35Audio7Channel4Typebb
// void Audio::CAudioRelay::PlaySound( const char*, const RwV3d*, Audio::Channel::Type, bool, bool ) { }

/*
====================
fn_8003A950
====================
*/
// 0x8003A950
extern "C" void fn_8003A950() {
    // TODO: wall (signature): fn_800380D4 (unit 0x80033D94, unreconstructed) takes the first parameter; the 0x100000 test and the two flags are unnamed.
}

/*
====================
fn_8003AA28
====================
*/
// 0x8003AA28
extern "C" void fn_8003AA28() {
    // TODO
}

/*
====================
unk_80039E40::fn_8003AC14
====================
*/
// 0x8003AC14
void unk_80039E40::fn_8003AC14() {
    // TODO
}

/*
====================
unk_80039E40::fn_8003AE68
====================
*/
// 0x8003AE68
void unk_80039E40::fn_8003AE68() {
    // TODO
}

/*
====================
unk_80039E40::fn_8003AE98
====================
*/
// 0x8003AE98
void unk_80039E40::fn_8003AE98() {
    // TODO
}

/*
====================
fn_8003AFDC
====================
*/
// 0x8003AFDC
extern "C" void fn_8003AFDC() {
    // TODO
}

/*
====================
fn_8003B000
====================
*/
// 0x8003B000
extern "C" void fn_8003B000() {
    // TODO
}

/*
====================
fn_8003B1B0
====================
*/
// 0x8003B1B0
extern "C" void fn_8003B1B0() {
    // TODO
}

/*
====================
fn_8003B244
====================
*/
// 0x8003B244
extern "C" void fn_8003B244() {
    // TODO
}

/*
====================
fn_8003B3C4
====================
*/
// 0x8003B3C4
extern "C" void fn_8003B3C4() {
    // TODO
}

/*
====================
fn_8003B3E0
====================
*/
// 0x8003B3E0
extern "C" void fn_8003B3E0() {
    // TODO
}

/*
====================
fn_8003B490
====================
*/
// 0x8003B490
extern "C" void fn_8003B490() {
    // TODO
}
