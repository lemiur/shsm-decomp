// Unit name and boundary: class CSequenceObject (sinit registration string, lane plan boundary)

#include <game/CSequenceObject.h>
#include <climax/Heap.h>

extern "C" void fn_80024B80( unk_80024158& self, int mode );
extern "C" void fn_80024B90( unk_80024158& self, int mode, u8 flag );

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803E9620 size 0x9 lbl_803E9620
//   .data 0x803E962C size 0xF0 jumptable_803E962C
//   .data 0x803E971C size 0x2B lbl_803E971C
//   .data 0x803E9748 size 0xF0 lbl_803E9748
//   .data 0x803E9838 size 0x10 lbl_803E9838
//   .data 0x803E9848 size 0x10 lbl_803E9848
//   .bss 0x8046E8D0 size 0x10 lbl_8046E8D0
//   .sbss 0x80529818 size 0x4 lbl_80529818
//   .sbss 0x8052981C size 0x4 lbl_8052981C
//   .sdata2 0x8052C378 size 0x4 lbl_8052C378
//   .sdata2 0x8052C37C size 0x4 lbl_8052C37C
//   .sdata2 0x8052C380 size 0x8 lbl_8052C380

/*
====================
CSequenceObject::CSequenceObject
====================
*/
// 0x800A5928
// Open: the vptr stores name __vt__15CSequenceObject, not lbl_803E9748 (the vtable is not modelled yet).
// fn_80024B80 takes the unk_80024158 base by reference: a pointer conversion from this would add a null check that retail does not emit.
CSequenceObject::CSequenceObject( const unk_AttributePacket& packet, bool flag ) :
    unk_8009D2E4( packet, flag ),
    field_0x154( 0 ),
    field_0x194( 0 ),
    field_0x198( 0.0f ),
    field_0x1A4( 0 ),
    field_0x1A8( -1 )
{
    fn_80024B80( *this, 2 );
    field_0x158 = fn_80025674( unk_80024158::fn_80024A14() );
    field_0x14C = 0;
    field_0x14D = 0;
    field_0x150 = 0;
}

/*
====================
unk_800A5A3C::unk_800A5A3C
====================
*/
// 0x800A5A3C
unk_800A5A3C::unk_800A5A3C() {
}

/*
====================
unk_800A5A3C::~unk_800A5A3C
====================
*/
// 0x800A5A74
unk_800A5A3C::~unk_800A5A3C() {
}

/*
====================
fn_800A5ADC
====================
*/
// 0x800A5ADC
extern "C" void fn_800A5ADC() {
    // TODO
}

/*
====================
fn_800A5D38
====================
*/
// 0x800A5D38
extern "C" void fn_800A5D38() {
    // TODO
}

/*
====================
fn_800A6020
====================
*/
// 0x800A6020
// Slot function of the CSequenceObject vtable (not reconstructed yet): the base CAttributeHandler
// method is called directly, then the unk_80024158 subobject at 0x3C with the flag byte at 0x14D.
void CSequenceObject::fn_800A6020() {
    CAttributeHandler::fn_801A7A88();
    fn_80024B90( *this, 1, field_0x14D );
}

/*
====================
fn_800A605C
====================
*/
// 0x800A605C
extern "C" void fn_800A605C() {
    // TODO
}

/*
====================
fn_800A65B0
====================
*/
// 0x800A65B0
void fn_800A65B0( const char*, ... ) {
    // TODO
}

/*
====================
fn_800A6600
====================
*/
// 0x800A6600
extern "C" void fn_800A6600() {
    // TODO
}

/*
====================
fn_800A69E4

Factory of the registry entry lbl_8052981C; address taken by the static initialiser.
====================
*/
// 0x800A69E4
CAttributeHandler* fn_800A69E4( const unk_AttributePacket& packet ) {
    return new( GAME_HEAP, GAME_HEAP_FLAGS ) CSequenceObject( packet, true );
}

/*
====================
fn_800A6A30
====================
*/
// 0x800A6A30
extern "C" void fn_800A6A30() {
    // TODO
}

/*
====================
fn_800A6A38
====================
*/
// 0x800A6A38
extern "C" void fn_800A6A38() {
    // TODO
}

/*
====================
fn_800A6A40
====================
*/
// 0x800A6A40
extern "C" void fn_800A6A40() {
    // TODO
}

/*
====================
fn_800A6A48
====================
*/
// 0x800A6A48
extern "C" void fn_800A6A48() {
    // TODO
}

/*
====================
fn_800A6A50
====================
*/
// 0x800A6A50
extern "C" void fn_800A6A50() {
    // TODO
}

/*
====================
fn_800A6A58
====================
*/
// 0x800A6A58
extern "C" void fn_800A6A58() {
    // TODO
}

// Compiler-generated, no source of their own: fn_800A6984 (0x800A6984).
