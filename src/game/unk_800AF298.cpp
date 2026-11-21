// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800AF298.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803EA0F8 size 0x5C lbl_803EA0F8
//   .data 0x803EA154 size 0x1C lbl_803EA154
//   .data 0x803EA170 size 0x68 lbl_803EA170
//   .data 0x803EA1D8 size 0x2B lbl_803EA1D8
//   .data 0x803EA204 size 0x26 lbl_803EA204
//   .data 0x803EA22C size 0x26 lbl_803EA22C
//   .data 0x803EA254 size 0xB8 lbl_803EA254
//   .data 0x803EA30C size 0x64 lbl_803EA30C
//   .data 0x803EA370 size 0x98 __vt__Q25Movie13CMovieTrigger
//   .bss 0x8046EA80 size 0xC lbl_8046EA80
//   .bss 0x8046EA8C size 0xC lbl_8046EA8C
//   .sdata 0x80527358 size 0x4 lbl_80527358
//   .sdata 0x8052735C size 0x7 lbl_8052735C
//   .sdata 0x80527364 size 0x4 lbl_80527364
//   .sdata 0x80527368 size 0x4 lbl_80527368
//   .sdata 0x8052736C size 0x4 lbl_8052736C
//   .sdata 0x80527370 size 0x4 lbl_80527370
//   .sdata 0x80527374 size 0x7 lbl_80527374
//   .sbss 0x80529890 size 0x4 lbl_80529890
//   .sbss 0x80529894 size 0x4 lbl_80529894
//   .sbss 0x80529898 size 0x4 lbl_80529898
//   .sbss 0x8052989C size 0x4 lbl_8052989C
//   .sdata2 0x8052C448 size 0x4 lbl_8052C448
//   .sdata2 0x8052C44C size 0x4 lbl_8052C44C
//   .sdata2 0x8052C450 size 0x8 lbl_8052C450

/*
====================
Movie::CMovieTrigger::~CMovieTrigger
====================
*/
// 0x800AF298
Movie::CMovieTrigger::~CMovieTrigger() {
    fn_801A9E00( msg );
    fn_801A9940( msg );
}

/*
====================
Movie::CMovieTrigger::fn_801A73A8

Reads the commands of the attribute packet. Command ids 0, 1 and 2 are not named by any evidence
yet: id 0 takes the message name, id 1 keeps the payload pointer and id 2 keeps the flag that is
clear when the payload is non-zero. Member names invented.
====================
*/
// 0x800AF32C
void Movie::CMovieTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529890 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case 0:
            fn_801AA218( msg, it->GetString(), 0 );
            break;
        case 1:
            field_0x3c = it->GetString();
            break;
        case 2:
            field_0x40 = it->GetU32() == 0;
            break;
        }
        ++it;
    }
}

/*
====================
Movie::CMovieTrigger::fn_8009CB1C

Posts the trigger's payload when the delivered message is the trigger's own message: on the
message lbl_80529900 when field_0x40 is set, on lbl_80529908 otherwise. Names invented.
====================
*/
// 0x800AF3FC
void Movie::CMovieTrigger::fn_8009CB1C( unk_Msg& delivered ) {
    if ( delivered.record == msg.record ) {
        if ( field_0x40 == true ) {
            PostMsg( lbl_80529900, (void*)field_0x3c );
        } else {
            PostMsg( lbl_80529908, (void*)field_0x3c );
        }
    }
}

/*
====================
fn_800AF498
====================
*/
// 0x800AF498
extern "C" void fn_800AF498() {
    // TODO
}

/*
====================
fn_800AF79C
====================
*/
// 0x800AF79C
extern "C" void fn_800AF79C() {
    // TODO
}

/*
====================
fn_800AFA04
====================
*/
// 0x800AFA04
extern "C" void fn_800AFA04() {
    // TODO
}

/*
====================
fn_800AFAB4
====================
*/
// 0x800AFAB4
extern "C" void fn_800AFAB4() {
    // TODO
}

/*
====================
fn_800AFB40
====================
*/
// 0x800AFB40
extern "C" void fn_800AFB40() {
    // TODO
}

/*
====================
fn_800AFB44
====================
*/
// 0x800AFB44
extern "C" void fn_800AFB44() {
    // TODO
}

/*
====================
fn_800AFDCC
====================
*/
// 0x800AFDCC
extern "C" void fn_800AFDCC() {
    // TODO
}

/*
====================
fn_800AFE7C
====================
*/
// 0x800AFE7C
extern "C" void fn_800AFE7C() {
    // TODO
}

/*
====================
fn_800B012C
====================
*/
// 0x800B012C
extern "C" void fn_800B012C() {
    // TODO
}

/*
====================
fn_800B01B0
====================
*/
// 0x800B01B0
extern "C" void fn_800B01B0() {
    // TODO
}

/*
====================
fn_800B05FC
====================
*/
// 0x800B05FC
extern "C" void fn_800B05FC() {
    // TODO
}

/*
====================
fn_800B0B80

Address taken by the static initialiser: probably an inline factory that belongs in the header.
====================
*/
// 0x800B0B80
extern "C" void fn_800B0B80() {
    // TODO
}

/*
====================
Movie::CMovieTrigger::MakeNew

Address taken by the static initialiser: probably an inline factory that belongs in the header.
====================
*/
// 0x800B0BFC
// TODO: signature not expanded or needs a complete type: MakeNew__Q25Movie13CMovieTriggerFRCQ23RWS16CAttributePacket
// void Movie::CMovieTrigger::MakeNew( const RWS::CAttributePacket& ) { }

/*
====================
fn_800B0C44
====================
*/
// 0x800B0C44
extern "C" void fn_800B0C44() {
    // TODO
}

/*
====================
fn_800B0C4C
====================
*/
// 0x800B0C4C
extern "C" void fn_800B0C4C() {
    // TODO
}

/*
====================
fn_800B0C54
====================
*/
// 0x800B0C54
extern "C" void fn_800B0C54() {
    // TODO
}

/*
====================
fn_800B0C5C
====================
*/
// 0x800B0C5C
extern "C" void fn_800B0C5C() {
    // TODO
}

// Compiler-generated, no source of their own: fn_800B0AD8 (0x800B0AD8).
