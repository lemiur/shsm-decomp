// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8009D2E4.h>
#include <game/unk_803E8E64.h>
#include <game/unk_80024158.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .sdata2 0x8052B7A0 size 0x8 lbl_8052B7A0

/*
====================
unk_8009D2E4::fn_8009ECB0
====================
*/
// Copies the position from the C-linkage accessor fn_8009CD14 into the 16-byte out vector.
// The w word is written as zero. The signature is from the retail arguments (r4 is the out
// pointer); the name of the out type (unk_Vec4) is invented.
// 0x800174B8
void unk_8009D2E4::fn_8009ECB0( unk_Vec4* out ) {
    const unk_RwV3d* pos = fn_8009CD14( this );
    out->x = pos->x;
    out->y = pos->y;
    out->z = pos->z;
    out->w = 0.0f;
}

/*
====================
unk_803E8E64::fn_80017504
====================
*/
// 0x80017504
bool unk_803E8E64::fn_80017504() {
    return true;
}

/*
====================
unk_803E8E64::fn_8001750C
====================
*/
// 0x8001750C
void unk_803E8E64::fn_8001750C() {
}

/*
====================
unk_803E8E64::fn_80017510
====================
*/
// 0x80017510
void unk_803E8E64::fn_80017510() {
}

/*
====================
unk_803E8E64::fn_80017514
====================
*/
// 0x80017514
void unk_803E8E64::fn_80017514() {
}

/*
====================
unk_803E8E64::fn_80017518
====================
*/
// 0x80017518
void unk_803E8E64::fn_80017518() {
}

/*
====================
unk_803E8E64::fn_8001751C
====================
*/
// 0x8001751C
bool unk_803E8E64::fn_8001751C() {
    return false;
}

/*
====================
unk_803E8E64::fn_80017524
====================
*/
// 0x80017524
bool unk_803E8E64::fn_80017524() {
    return false;
}

/*
====================
unk_803E8E64::fn_8001752C
====================
*/
// 0x8001752C
bool unk_803E8E64::fn_8001752C() {
    return false;
}

/*
====================
unk_803E8E64::fn_80017534
====================
*/
// 0x80017534
bool unk_803E8E64::fn_80017534() {
    return true;
}

/*
====================
unk_803E8E64::fn_8001753C
====================
*/
// 0x8001753C
bool unk_803E8E64::fn_8001753C() {
    return false;
}

/*
====================
unk_803E8E64::fn_80017544
====================
*/
// 0x80017544
bool unk_803E8E64::fn_80017544() {
    return true;
}

/*
====================
unk_803E8E64::fn_8001754C
====================
*/
// 0x8001754C
bool unk_803E8E64::fn_8001754C() {
    return true;
}

/*
====================
unk_80024158::fn_80017554
====================
*/
// 0x80017554
void unk_80024158::fn_80017554() {
}

/*
====================
unk_80024158::fn_80017558
====================
*/
// 0x80017558
void unk_80024158::fn_80017558() {
}

/*
====================
unk_80024158::fn_8001755C
====================
*/
// 0x8001755C
void unk_80024158::fn_8001755C() {
}

/*
====================
unk_8009D2E4::fn_80017560
====================
*/
// 0x80017560
void unk_8009D2E4::fn_80017560() {
}

/*
====================
unk_8009D2E4::fn_80017564
====================
*/
// 0x80017564
void unk_8009D2E4::fn_80017564() {
}

/*
====================
unk_8009D2E4::fn_80017568
====================
*/
// 0x80017568
u32 unk_8009D2E4::fn_80017568() {
    return field_0x138;
}
