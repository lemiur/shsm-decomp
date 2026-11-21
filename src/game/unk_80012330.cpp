// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80012330.h>
#include <rwsdk/badevice.h>

/*
====================
fn_80012330
====================
*/
// 0x80012330
extern "C" void fn_80012330( unk_RefBlock* block, void* owner ) {
    if ( --block->refCount <= 0 ) {
        if ( block != NULL ) {
            RwFree( block->rwData );
            delete block;
        }
    }
}
