// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80015358.h>

/*
====================
fn_80015358

Appends { a, b } to lbl_8045A7E0 at the running count, then adds b to lbl_805291C4 and a * b to
lbl_805291C8. The parameter names and the sums' meanings are invented.
====================
*/
// 0x80015358
extern "C" void fn_80015358( int a, int b ) {
    int index = lbl_805291B8;
    lbl_8045A7E0[ index ].value = a;
    lbl_8045A7E0[ index ].count = b;
    lbl_805291B8 = index + 1;
    lbl_805291C4 += b;
    lbl_805291C8 += a * b;
}
