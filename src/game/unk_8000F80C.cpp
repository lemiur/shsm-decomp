// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8000F80C.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .sdata 0x80526810 size 0x6 lbl_80526810
//   .sdata 0x80526818 size 0x6 lbl_80526818

/*
====================
unk_8000F80C::unk_8000F80C
====================
*/
// 0x8000F80C
unk_8000F80C::unk_8000F80C( unk_8000F80CSource* source, u32 arg, void* owner ) {
    name = NULL;
    nameHash = 0;
    flags = 2;
    fn_80012904( &lbl_8045A6BC, source, this );
    field_0x14 = owner;
    if ( source->name != NULL ) {
        name = source->name;
    }
    nameHash = RWS::SHUtils::GetStringHashForceLower( name, -1 );
    field_0x8 = arg;
    field_0xC = source->data->field_0xE;
    unk_8000F80CHolder* holder = fn_80188218( source, "Flags" );
    if ( holder == NULL ) {
        flags = 2;
    } else {
        flags = ( *fn_80189D10( holder, "Flags" ) )->flags;
    }
}

/*
====================
unk_8000F80C::GetName
====================
*/
// 0x8000F8E8
const char* unk_8000F80C::GetName() const {
    return name;
}

/*
====================
unk_8000F80C::GetNameHash
====================
*/
// 0x8000F8F0
const u32& unk_8000F80C::GetNameHash() const {
    return nameHash;
}
