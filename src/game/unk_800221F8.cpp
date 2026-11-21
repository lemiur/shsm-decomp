// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800221F8.h>
#include <rwsdk/plugin/dmorph/rpdmorph.h>

/*
====================
fn_800221F8

Callback slot of a morph controller: the owner calls the slot as (context, atomic, deltaTime) and
the context is not used, so the slot forwards the atomic and the time to RpDMorphAtomicAddTime.
====================
*/
// 0x800221F8
extern "C" RpAtomic* fn_800221F8( void* context, RpAtomic* atomic, RwReal deltaTime ) {
    return RpDMorphAtomicAddTime( atomic, deltaTime );
}
