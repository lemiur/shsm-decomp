// MsgTrigger: class name from the registration strings lbl_803E9E20 and lbl_803E9E2C.
// The destructor precedes the static initialiser; the inline factory follows it in .text.

#include <game/MsgTrigger.h>

/*
====================
MsgTrigger::~MsgTrigger

Nothing to release: the base destructor does the work.
====================
*/
// 0x800AB158
MsgTrigger::~MsgTrigger() {
}

u32 lbl_80529858 = fn_801A7A84( "MsgTrigger" );
unk_801A8F6C lbl_8052985C( "MsgTrigger", fn_800AB210, sizeof( MsgTrigger ) );
