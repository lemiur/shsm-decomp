// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800999A8.h>
#include <game/Display.h>
#include <game/unk_800E671C.h>
#include <revolution/thp.h>

// Result of the last fn_8009B840 call (-1 on failure).
s32 lbl_8052976C;

/*
====================
fn_800999A8

Places the movie frame, then maps the movie player's state to a code: 1 for THP states 0 and 3, 2
for state 5, and 0 for states 1, 2 and 4 and for any other value. The THP state values are kept as
numbers; their names are not recovered.
====================
*/
// 0x800999A8
extern "C" s32 fn_800999A8() {
    s32 result;
    s32 state;

    // 16:9 display: a 640-wide frame at x 0; 4:3 display: an 853-wide frame at x -106.
    if ( lbl_80529E44->field_0x250 ) {
        lbl_8052976C = fn_8009B840( lbl_80478E78, 0, 0, 640, GetDisplayHeight(), 0.0f );
    } else {
        lbl_8052976C = fn_8009B840( lbl_80478E78, -106, 0, 853, GetDisplayHeight(), 0.0f );
    }

    THPPlayerDrawDone();
    result = 0;
    state = THPPlayerGetState();
    switch ( state ) {
    case 1:
    case 2:
    case 4:
        result = 0;
        break;
    case 0:
    case 3:
        result = 1;
        break;
    case 5:
        result = 2;
        break;
    }
    return result;
}

/*
====================
fn_80099A78
====================
*/
// 0x80099A78
extern "C" void fn_80099A78() {
}
