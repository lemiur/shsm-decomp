// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.
// Parked (NonMatching): the four bodies are exact, but the class declarations are partial and
// R13 is open at unk_800B1754 slots 0x68 and 0x6C (overrides of its second base).

#include <game/unk_800B28E0.h>
#include <game/unk_800B1754.h>
#include <game/unk_800B2E58.h>

void* lbl_805273C8 = 0;

/*
====================
unk_800B1754::fn_800B28E0
====================
*/
// 0x800B28E0
void* unk_800B1754::fn_800B28E0() {
    return &lbl_805273C8;
}

/*
====================
unk_800B1754::fn_800B28E8
====================
*/
// 0x800B28E8
u8 unk_800B1754::fn_800B28E8() {
    return field_0x1C;
}

/*
====================
unk_800B1754::fn_800B28F0
====================
*/
// 0x800B28F0
bool unk_800B1754::fn_800B28F0() {
    return field_0x8 != 0;
}

/*
====================
unk_800B2E58::fn_800B2904
====================
*/
// 0x800B2904
bool unk_800B2E58::fn_800B2904() {
    return false;
}
