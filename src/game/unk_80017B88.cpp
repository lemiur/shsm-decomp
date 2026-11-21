// Unit name and boundary: unit name unconfirmed (address form; lane 0103 unit plan, no sibling object boundary)

#include <game/unk_80017B88.h>
#include <game/unk_8001777C.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803E0A54 size 0xC lbl_803E0A54 (the vtable of unk_80017E6C, compiler-generated)

/*
====================
unk_80017E6C::unk_80017E6C
====================
*/
// 0x80017E6C
unk_80017E6C::unk_80017E6C() {
    field_0x4 = 0;
    field_0x8 = NULL;
}

/*
====================
unk_80017E6C::~unk_80017E6C
====================
*/
// 0x80017B88
unk_80017E6C::~unk_80017E6C() {
    if (field_0x8 != NULL) {
        delete field_0x8;
        field_0x8 = NULL;
    }
}

/*
====================
Animation::s_MountRef::~s_MountRef
====================
*/
// 0x80017C00
Animation::s_MountRef::~s_MountRef() {
}

/*
====================
unk_8001777C::fn_8000CFD4
====================
*/
// 0x80017C5C
// TODO: body calls three virtual slots (0x10C, 0x110, 0x114) of CBaseBehaviour's vtable; no declared slot
void unk_8001777C::fn_8000CFD4() {
    // TODO
}

/*
====================
unk_8001777C::fn_80017CBC
====================
*/
// 0x80017CBC
// TODO: body not reconstructed (allocates through operator new and __nwa__ of unk_80017E6C arrays)
void unk_8001777C::fn_80017CBC() {
    // TODO
}

/*
====================
unk_8001777C::fn_80017DD8
====================
*/
// 0x80017DD8
// TODO: stores the new 0xC object at field 0x14C; unk_8001777C.h declares field_0x148 as the AnimController
void unk_8001777C::fn_80017DD8() {
    // TODO
}
