// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80099A7C.h>

/*
====================
fn_80099A7C

Stops the movie player and frees its work buffer.
====================
*/
// 0x80099A7C
extern "C" void fn_80099A7C() {
    fn_8009B37C();
    THPPlayerClose();
    delete[] lbl_80529768;
    lbl_80529768 = NULL;
    THPPlayerQuit();
}
