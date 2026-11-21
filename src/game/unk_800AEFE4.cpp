// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800AEFE4.h>
#include <game/CPreCache.h>

u8 lbl_80529888[8];

/*
====================
fn_800AEFE4
====================
*/
/*
 * Requests the asset named by name under the owner key lbl_80529888, with no loader callback, and
 * sets its priority to zero. Returns the request handle. The second argument is not read: every
 * caller passes 1. Name invented.
 */
// 0x800AEFE4
extern "C" void* fn_800AEFE4( const char* name, int unused, int flags ) {
    void* handle = fn_801B05C0( lbl_8052A5C0, name, (unk_SpawnerResource*)lbl_80529888, flags, NULL, NULL );
    fn_801B2380( lbl_8052A5C0, handle, 0.0f );
    return handle;
}
