// unit name unconfirmed: a movie-frame draw that waits on lbl_8046BB40; the file name is invented.

#include <game/unk_MovieFrame.h>
#include <revolution/os/OSThread.h>
#include <revolution/os/OSTime.h>

/*
====================
fn_8009B840
====================
*/
// 0x8009B840
extern "C" int fn_8009B840( unk_MovieSurface* surface, int x, int y, int width, int height, float depth ) {
    int tries = 10;
    while ( lbl_8046BB40.frame == NULL && tries > 0 ) {
        OSSleepTicks( OSMillisecondsToTicks( (OSTime)10 ) );
        tries--;
    }

    if ( lbl_8046BB40.field_0xA0 != 0 && lbl_8046BB40.field_0xA4 != 0 && lbl_8046BB40.frame != NULL ) {
        fn_8009A024( surface );
        fn_8009A4CC( lbl_8046BB40.frame->field_0x0, lbl_8046BB40.frame->field_0x4, lbl_8046BB40.frame->field_0x8,
                     depth, x, y, lbl_8046BB40.field_0x80, lbl_8046BB40.field_0x84, width, height );
        RestoreGXState();
        return ( lbl_8046BB40.frame->field_0xC + lbl_8046BB40.field_0xC0 ) % lbl_8046BB40.field_0x50;
    }
    return -1;
}
