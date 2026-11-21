// unk_8002D148: unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.
//
// Both functions are slots of the vtable lbl_803E1788 (size 0x1F8, a CPointOfInterest-derived class
// whose identity is not reconstructed). fn_8002D148 returns a float constant in f1. fn_8002D150 returns
// 0 in r3: the int is a guess, since the bytes do not fix the slot's return type.

#include <game/unk_8002D148.h>

/*
====================
fn_8002D148
====================
*/
// 0x8002D148
extern "C" float fn_8002D148() {
    return 1.6f;
}

/*
====================
fn_8002D150
====================
*/
// 0x8002D150
extern "C" int fn_8002D150() {
    return 0;
}
