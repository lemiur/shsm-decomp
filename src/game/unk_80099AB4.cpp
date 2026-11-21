// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80099AB4.h>
#include <climax/Heap.h>

// Data owned by this unit (declared in include/game/unk_80099AB4.h):
//   .bss 0x8046A790 size 0x10 lbl_8046A790: video info filled by THPPlayerGetVideoInfo
//   .bss 0x8046A7A0 size 0x10 lbl_8046A7A0: audio info filled by THPPlayerGetAudioInfo
//   .sbss 0x80529768 size 0x4 lbl_80529768: decode buffer, shared with fn_80099A7C
THPVideoInfo   lbl_8046A790;
THPAudioInfo   lbl_8046A7A0;
u8*            lbl_80529768;

/*
====================
fn_80099AB4
====================
*/
// 0x80099AB4
extern "C" u8* fn_80099AB4( const char* path, u32 flags ) {
    fn_8009A730( 1 );
    if ( fn_8009AA00( path, 0 ) ) {
        THPPlayerGetVideoInfo( &lbl_8046A790 );
        THPPlayerGetAudioInfo( &lbl_8046A7A0 );
        // The operator is called directly: the array-new expression adds a size multiply that
        // the retail code does not have. Same arguments: size, GAME_HEAP, GAME_HEAP_FLAGS.
        lbl_80529768 = (u8*) operator new[]( fn_8009ACC8(), GAME_HEAP, GAME_HEAP_FLAGS );
        fn_8009AD80( lbl_80529768 );
        s32 loop = 0;
        if ( flags & 1 ) {
            loop = 1;
        }
        THPPlayerPrepare( 0, loop, 0 );
        THPPlayerPlay();
    } else {
        fn_8009B37C();
        THPPlayerClose();
        delete[] lbl_80529768;
        lbl_80529768 = NULL;
        THPPlayerQuit();
    }
    return lbl_80529768;
}
